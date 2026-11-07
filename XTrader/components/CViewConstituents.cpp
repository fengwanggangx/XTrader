#include "CViewConstituents.h"
#include "CMarketPageController.h"
#include "ui_CViewConstituents.h"

#include <QHeaderView>

CViewConstituents::CViewConstituents(QWidget* pParent) : QWidget(pParent), m_ui(std::make_unique<Ui::CViewConstituentsClass>())
{
}

void CViewConstituents::Initialize(CTraderServices& services)
{
	if (nullptr != m_pServices)
	{
		return;
	}
	m_pServices = &services;
	m_ui->setupUi(this);
	m_ui->table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	m_ui->table->sortByColumn(-1, Qt::AscendingOrder);
	m_ui->table->SetSearchColumns({ 0, 1 });
	connect(m_ui->searchEdit, &QLineEdit::textChanged, m_ui->table, &CUITable::Search);
	connect(m_ui->searchEdit, &QLineEdit::returnPressed, this, [this]()
			{ m_ui->table->Search(m_ui->searchEdit->text()); });
	connect(m_ui->searchButton, &QPushButton::clicked, this, [this]()
			{ m_ui->table->Search(m_ui->searchEdit->text()); });
	connect(m_ui->table, &CUITable::ResultsChanged, this, [this](int nCount)
			{ m_ui->countLabel->setText((m_strSector.isEmpty() ? QString() : m_strSector + " · ") + QString("共%1只").arg(nCount)); });
	m_controller = std::make_unique<CMarketPageController>(*m_pServices, MarketTableMode::Constituents, m_ui->table, this);
}

CViewConstituents::~CViewConstituents() = default;

void CViewConstituents::SetSector(const CSectorInfo& sector, const std::vector<CSecurity>& securities)
{
	m_strSector = QString::fromStdString(sector.m_strName);
	m_controller->SetConstituents(securities);
}
